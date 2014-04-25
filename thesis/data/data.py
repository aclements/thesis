# This implementation is lame, but the interface is designed to
# support a great deal of optimization.  Since we record the operator
# tree and only realize it when forced to materialize the result, we
# could perform expression optimization.  We could almost certainly
# reduce overhead by making the operators deal with blocks of tuples,
# rather than individual tuples.  We could generate Python code to
# implement the operator tree and eval it.  We could probably optimize
# for tuple shape, since there are likely to be only a few different
# shapes.  We could push various inner loops into C (especially if we
# work with tuple blocks).  If I generate Python code, I might not
# need batching since I could process individual tuples through
# without per-operation overheads.

__all__ = ["AbstractTable", "Table", "SelectionFilter",
           "DiscretePropStat", "pinject"]

import re
import collections

# XXX R's data frames distinguish categorical and continuous variables
# and treat these very differently (e.g., ggplot2 automatically
# separates groups on all categorical variables).  Categorical data is
# represented as "factors" which, as far as I can tell, are basically
# symbols.  I could simply say that strings are categorical and
# numbers are not and use stringified numbers when I want numbers to
# be categorical.  This approach works less well when my data is
# heterogeneous, but I could homogenize it by considering properties
# with the same name but different types to be different properties.
# If I were to dump all of the Linux kernel config into this, would
# that be a disaster, or would it make sense for all of that to be
# categorical?

class Schema(object):
    __slots__ = ["__propNames", "__propNameSet"]

    def __init__(self, propNames):
        self.__propNames = tuple(propNames)
        self.__propNameSet = set(self.__propNames)

    def __eq__(self, o):
        return isinstance(o, Schema) and o.__propNames == self.__propNames

    def __hash__(self):
        return hash(self.__propNames)

    def checkProp(self, propName):
        """Check that propName is a known property name.  Throws
        ValueError if it is not."""
        if propName not in self.__propNameSet:
            raise ValueError("Unknown property name %r" % propName)

    def names(self):
        """Return a tuple of property names, in order."""
        return self.__propNames

class AbstractTable(object):
    __slots__ = ["__schema"]

    def __init__(self, schema):
        self.__schema = schema

    def __getitem__(self, index):
        """If index is a string, return an iterator over the property
        called 'index'."""
        if isinstance(index, basestring):
            self.__schema.checkProp(index)
            for row in self:
                yield row[index]
        else:
            raise ValueError("Bad item spec %r" % index)

    def filter(self, *preds):
        """Filter this data source by calling each of preds on each
        row and yielding only those for which pred returns a true
        value.  pred can also be a pair (string, val), which will
        match if the named property has the given val."""
        if len(preds) == 0:
            return self

        # XXX Could optimize if there's just one pred and it's a
        # function
        parts = []
        flocals = {}
        for i, pred in enumerate(preds):
            if isinstance(pred, collections.Callable):
                flocals["f%d" % i] = pred
                parts.append("f%d(row)" % i)
            elif isinstance(pred, tuple) and len(pred) == 2 and \
                    isinstance(pred[0], basestring):
                name, val = pred
                self.__schema.checkProp(name)
                flocals["v%d" % i] = val
                parts.append("row[%r] == v%d" % (name, i))
            else:
                raise ValueError("Bad filter spec %r" % pred)
        pfunc = eval("lambda row: %s" % " and ".join(parts), flocals)
        return _Op(self.__schema, self.__op_filter, [self], pfunc)

    @staticmethod
    def __op_filter(children, pfunc):
        for row in children[0]:
            if pfunc(row):
                yield row

    def map(self, *specs):
        """Transform each row of this table.

        Returns a dynamic table where each row is computed from a row
        of this table according to 'specs'.

        Each spec must be one of:
        * "*" - Copy all properties to the output.
        * string - Copy the corresponding property to the output.
        * (string, func) - Apply func to each row and record its
          result in the named property.
        * func - Apply func to each row and record its result in a
          property named "$"+func.__name__."""
        # XXX Could optimize for maps that simply permute the schema
        # XXX This makes it a pain to write a transformer that just
        # takes a row and computed a new whole row (e.g., adding
        # multiple columns)
        parts = []
        flocals = {}
        # XXX Very similar to group_by specs.  Can these be unified?
        for i, spec in enumerate(specs):
            if spec == "*":
                for col in self.__schema.names():
                    parts.append((col, "row[%r]" % col))
            elif isinstance(spec, basestring):
                self.__schema.checkProp(spec)
                parts.append((spec, "row[%r]" % spec))
            elif isinstance(spec, collections.Callable):
                flocals["f%d" % i] = spec
                parts.append(("$" + spec.__name__, "f%d(row)" % i))
            elif (isinstance(spec, tuple) and len(spec) == 2
                  and isinstance(spec[0], basestring)
                  and isinstance(spec[1], collections.Callable)):
                flocals["f%d" % i] = spec[1]
                parts.append((spec[0], "f%d(row)" % i))
            else:
                raise ValueError("Bad map spec %r" % spec)
        func = eval("lambda row: {%s}" % ",".join("%r:%s" % p for p in parts),
                    flocals)
        schema = Schema([part[0] for part in parts])
        return _Op(schema, self.__op_map, [self], func)

    @staticmethod
    def __op_map(children, func):
        for row in children[0]:
            yield func(row)

    def prop(self, name, default=None, omit=False):
        """Yield a sequence of the values for property 'name'.  If
        omit is True, then rows without a value for 'name' will be
        omitted.  If omit is False (the default), then the value of
        default will be substituted for missing properties."""
        self.__schema.checkProp(name)
        if omit:
            for row in self:
                val = row[name]
                if val is not None:
                    yield val
        elif default is None:
            for row in self:
                yield row[name]
        else:
            for row in self:
                val = row[name]
                yield default if val is None else val

    def viable_values(self, selectionFilter, extraProps=[]):
        """Return a map from property name to viable values based on
        an initial selection filter.  extraProps is a list of
        properties to compute statistics for, but which do not filter
        the data.

        The viable values of a property are the values of that
        property when the data set has been filtered by the selection
        filter on all of the *other* properties.  The viable value set
        has various nice properties when used for interactive
        filtering: changing the filter for one property never changes
        its viable value set (keeping the UI stable) and filtering
        down a property eliminates choices from other properties that
        wouldn't add anything to the result set."""

        # XXX Also a list of high-cardinality properties and how many
        # bins to summarize into?  Maybe a mapping from properties to
        # statistic type?  Could subsume extraProps.  If the table
        # knows the difference between categorical and non-categorical
        # variables, I could use that to distinguish.

        # XXX This could be replaced with a viable filter method that
        # works like filter, but for near matches, includes a tuple
        # containing only the missed property.  This could then be
        # passed to a general statistics method (though its statistics
        # for None would be wrong).
        return selectionFilter._viable_values(self, extraProps)

    def properties(self, omit=False):
        s = Schema(["name"])
        if omit:
            return _Op(s, self.__op_properties, [self])
        else:
            return Table([{"name": name} for name in self.__schema.names()],
                         s)

    @staticmethod
    def __op_properties(children):
        # Begin by assuming that all properties are omitted
        toOmit = set(children[0].__schema.names())
        keep = []
        # Scan the rows and check all properties that are omitted so
        # far to see if we should keep any of them.
        for row in children[0]:
            for k in toOmit:
                if row[k] is not None:
                    keep.append(k)
            if keep:
                toOmit.difference_update(keep)
                # Have we already found all of the properties in the
                # schema?
                if not toOmit:
                    break
                # In the common case, a row doesn't have any
                # properties we haven't seen before, so we only reset
                # the keep list when it actually gets used.
                keep = []
        for prop in children[0].__schema.names():
            if prop not in toOmit:
                yield {"name": prop}

    def __resolve_projection(self, proj):
        if isinstance(proj, basestring):
            proj = [proj]
        if isinstance(proj, (list, tuple)):
            all(self.__schema.checkProp(p) for p in proj)
            expr = "(%s)" % "".join("row[%r]," % p for p in proj)
            return eval("lambda row: " + expr), Schema(proj)
        if isinstance(proj, collections.Callable):
            return lambda row: (proj(row),), Schema(["$val"])
        raise ValueError("projection must be string, list, tuple, or callable")

    def group_by(self, proj, *specs):
        """Group this table by equivalence of a projection.

        Returns a dynamic table that contains one row for each
        distinct value of 'proj' applied to the rows of this table.
        Each output row consists of the properties projected by
        'proj', plus new properties defined by 'specs' and derived
        from the table of rows contributing to that output row.

        Each spec must be one of:
        * string - Record the group table as the named property.
        * func - Apply func to the group table and record the result
          in a property named "$"+func.__name__.
        * (string, func) - Apply func to the group table and record
          the result in the named property.

        The returned table is in the order that the first row in each
        group appeared in.  The table passed to reducer functions is
        in the same order as the rows in this table."""

        proj, projSchema = self.__resolve_projection(proj)
        reducers = []
        for spec in specs:
            if isinstance(spec, basestring):
                reducers.append((spec, lambda x:x))
            elif isinstance(spec, collections.Callable):
                reducers.append(("$" + spec.__name__, spec))
            elif (isinstance(spec, tuple) and len(spec) == 2
                  and isinstance(spec[0], basestring)
                  and isinstance(spec[1], collections.Callable)):
                reducers.append(spec)
            else:
                raise ValueError("Bad reducer spec %r" % (spec,))
        newProps = tuple(reducer[0] for reducer in reducers)
        schema = Schema(projSchema.names() + newProps)
        return _Op(schema, self.__op_group_by, [self], proj, projSchema,
                   reducers)

    @staticmethod
    def __op_group_by(children, proj, projSchema, reducers):
        groups = collections.OrderedDict()
        for row in children[0]:
            key = proj(row)
            try:
                groups[key].append(row)
            except KeyError:
                groups[key] = [row]
        # Apply reducers and produce output rows
        for key, rows in groups.iteritems():
            group = Table(rows, children[0].__schema)
            # XXX Could build dict directly with compiled code
            result = {}
            for i, prop in enumerate(projSchema.names()):
                result[prop] = key[i]
            for prop, reducer in reducers:
                result[prop] = reducer(group)
            yield result

    def ungroup(self, prop):
        """Explode the sub-tables under prop."""
        self.__schema.checkProp(prop)
        newProps = list(self.__schema.names())
        pidx = newProps.index(prop)
        # Get one row to get prop's schema
        try:
            row = iter(self).next()
        except StopIteration:
            raise ValueError("Cannot ungroup an empty table")
        newProps[pidx:pidx+1] = row[prop].__schema.names()
        schema = Schema(newProps)
        return _Op(schema, self.__op_ungroup, [self], prop)

    @staticmethod
    def __op_ungroup(children, prop):
        for row in children[0]:
            subtable = row[prop]
            for subrow in subtable:
                nrow = row.copy()
                del nrow[prop]
                nrow.update(subrow)
                yield nrow

    def sort_by(self, proj):
        return _Op(self.__schema, self.__op_sort_by, [self],
                   self.__resolve_projection(proj)[0])

    @staticmethod
    def __op_sort_by(children, proj):
        return iter(sorted(children[0], key=proj))

    def append(self, *others):
        """Append other tables to this table.

        Returns a dynamic table that contains the rows of this table,
        followed by the rows of the first argument, then the next, and
        so on.  The tables must have identical schemata."""
        for o in others:
            if o.__schema != self.__schema:
                raise ValueError(
                    "Cannot append tables with different schemata")

        return _Op(self.__schema, self.__op_append, [self] + list(others))

    @staticmethod
    def __op_append(children):
        for child in children:
            for row in child:
                yield row

    def __str__(self):
        props = self.__schema.names()
        header = Table([{n: n for n in props}], self.__schema)
        # Stringify all cells and find their width/height
        def stringify(val):
            if val is None:
                s = ["N/A"]
            else:
                s = str(val).split("\n")
            h = len(s)
            w = max(len(line) for line in s) + 1
            return (s, w, h)
        data = Table(list(header.append(self).map(
                    *[(p, lambda row, p=p: stringify(row[p]))
                      for p in props])))
        # Compute width of each column
        cols = [(prop, max(v[1] for v in data.prop(prop) if v is not None))
                for prop in props]
        # Print rows
        parts = []
        for row in data:
            rowHeight = max(v[2] for v in row.values())
            for rowLine in range(rowHeight):
                for prop, width in cols:
                    lines = row[prop][0]
                    parts.append((lines[rowLine] if rowLine < len(lines)
                                  else "").ljust(width))
                parts.append("\n")
        # No terminating newline
        parts.pop()
        return "".join(parts)

class _Op(AbstractTable):
    __slots__ = ["__op", "__children", "__args"]

    def __init__(self, schema, op, children, *args):
        self.__op = op
        self.__children = children
        self.__args = args
        super(_Op, self).__init__(schema)

    def __iter__(self):
        return self.__op(self.__children, *self.__args)

class Table(AbstractTable):
    """A leaf table that wraps an existing Python iterator."""

    __slots__ = ["__data"]

    def __init__(self, data, schema=None):
        """Construct a table containing data.  If schema is None, data
        may be any iterator or iterable yielding a sequence of
        dictionaries and the schema will be deduced from the keys of
        the dictionaries.  The input dictionaries will be copied and
        "filled out" by setting any missing keys to None.

        If schema is provided, data must be an iterable object (not
        just an iterator) and must contain dictionaries with key sets
        that exactly match schema.  This will not copy the data or
        fill it out.  Unlike when schema in None, constructing a Table
        with a known schema is very fast."""

        if schema is None:
            # Deduce the schema from data and copy rows
            propSet = set()
            newData = []
            for row in data:
                propSet.update(row.iterkeys())
                # XXX Could avoid copying if sys.getrefcount(row) ==
                # 2, though achieving that is remarkably tricky.
                newData.append(row.copy())
            propNames = list(sorted(propSet, key=smart_key))
            schema = Schema(propNames)
            # Replace missing keys with None-valued keys
            for row in newData:
                for name in propNames:
                    row.setdefault(name)
            data = newData

        self.__data = data
        super(Table, self).__init__(schema)

    def __iter__(self):
        return iter(self.__data)

class SelectionFilter(object):
    """A selection filter is a filter of the form

        (prop1 in [a, b c]) and (prop2 in [d, e, f]) ..."""

    def __init__(self):
        self.__valMap = {}
        self.__rangeMap = {}
        self.__missing = {}

    def add(self, prop, accept):
        """Limit the filter to match rows whose prop is contained in
        accept (or has a non-empty intersection with vals for
        multi-valued properties).  By convention, a value of None in
        accept means to accept rows missing this property."""
        if prop in self.__valMap:
            self.__valMap[prop].intersection_update(accept)
        else:
            self.__valMap[prop] = set(accept)
        return self

    def addRange(self, prop, low, high, missing=False):
        """Limit the filter to match rows whose prop is in the range
        [low, high] (or has a non-empty intersection with this range,
        for multi-valued properties).  If missing is a true value,
        then accept rows that do not have this property."""
        if prop in self.__rangeMap:
            elow, ehigh = self.__rangeMap[prop]
            low = max(elow, low)
            high = min(ehigh, high)
        self.__rangeMap[prop] = (low, high)
        self.__missing[prop] = self.__missing.get(prop, True) and missing
        return self

    def __match(self, row, exact):
        """Returns True if row fully matches the filter, False if it
        fully misses.  If exact is True and row misses a single
        filter, returns the name of the property that missed."""

        miss = None

        # Handle range filters
        for prop, (low, high) in self.__rangeMap.iteritems():
            # Get value
            try:
                val = row[prop]
            except KeyError:
                if not self.__missing[prop]:
                    if exact or miss:
                        return False
                    miss = prop
                continue

            if isinstance(val, (set, list)):
                # Handle multivalued properties
                for subval in val:
                    if low <= subval <= high:
                        break
                else:
                    if exact or miss:
                        return False
                    miss = prop
            else:
                # XXX What if this is a distribution or something
                # where it's not just the Python value?  I suppose I
                # could overload the operators on that type.  That
                # trick wouldn't work for set filters.
                if not (low <= val <= high):
                    if exact or miss:
                        return False
                    miss = prop

        # Handle set filters
        for prop, accept in self.__valMap.iteritems():
            # Get value
            val = row[prop]

            if isinstance(val, (set, list)):
                if accept.isdisjoint(val):
                    if exact or miss:
                        return False
                    miss = prop
            else:
                if val not in accept:
                    if exact or miss:
                        return False
                    miss = prop

        return miss or True

    def __call__(self, row):
        return self.__match(row, exact=True)

    def _viable_values(self, data, extraProps):
        # The obvious implementation of this is to filter the data
        # several times, each time omitting one of the properties of
        # this selection filter, and collect the values of the omitted
        # property as the viable value set for that property.  That's
        # not what this does.  Instead, this uses a "two strikes"
        # algorithm.  It makes a single pass over the data.  Each
        # tuple that matches on all of the filtered properties
        # contributes matched values to all of the filtered
        # properties, just like it would for a normal filtered
        # statistics operation.  Additionally, each tuple that matches
        # on all but one filtered property contributes its value of
        # the property that failed the filter to the viable value set
        # for that property.

        varCountMap = {}
        for nameSet in [self.__valMap.keys(),
                        self.__rangeMap.keys(),
                        extraProps]:
            for name in nameSet:
                varCountMap[name] = collections.Counter()

        for row in data:
            m = self.__match(row, exact=False)
            if m is False:
                # Full miss
                continue
            # If full match, contribute to all of the filter stats.
            # If near match, contribute only to the missing filter's
            # stats.
            for prop in varCountMap.iterkeys() if (m is True) else [m]:
                val = row[prop]
                if isinstance(val, (set, list)):
                    for subval in val:
                        varCountMap[prop][subval] += 1
                else:
                    varCountMap[prop][val] += 1

        # Create final filter stats
        stats = {}
        for prop, counts in varCountMap.iteritems():
            stats[prop] = DiscretePropStat(counts)
        return stats

# XXX Should this just be a discrete distribution?  It doesn't
# strictly have anything to do with properties.
class DiscretePropStat(Table):
    """Statistics for a discretely-valued property.  This is a table
    where each row has value and count fields, sorted by value."""

    # XXX Should this just take the table?  Then it would be
    # repr-able.
    def __init__(self, counts):
        """Construct a statistics object for discrete, low-cardinality
        filters.  counts is a map from property value to the number of
        objects matching that value."""
        table = [{"value": k, "count": v} for k, v in counts.iteritems()]
        # XXX Use the original property's sorter instead of
        # hard-coding smart_key.
        table.sort(key=lambda row: smart_key(row["value"]))
        super(DiscretePropStat, self).__init__(
            table, Schema(["value", "count"]))

    def as_pairs(self):
        for row in self:
            yield (row["value"], row["count"])

#class BinnedPropStat(object):

def smart_key(value):
    # This attempts to extract numeric parts from value to make
    # numeric strings comparable to numbers and strings with numeric
    # parts ordered numerically instead of lexicographically.  The
    # idea is to split the value into a tuple of values.  Python 2.7
    # complicates this by deprecating comparisons of values with
    # unequal type, so for each piece, we use a pair of a type code
    # and the value so that values with distinct types will by sorted
    # by their type code and never directly compared.
    if isinstance(value, (int,long,float)):
        # Return a single piece so these can be sorted relative to
        # numeric strings.  We use int's type code because numeric
        # types *are* comparable.
        return ((id(int), value),)
    if isinstance(value, basestring):
        # Split into numbers, words, and punctuation
        return tuple((id(int), int(x)) if x.isdigit() else (id(type(value)), x)
                     for x in
                     re.findall("[0-9]+|[a-zA-Z]+|[^0-9a-zA-Z]+", value))
    if isinstance(value, (list, tuple)):
        return tuple((id(type(s)), smart_key(s)) for s in value)
    # Return anything else as a single piece.
    return ((id(type(value)), value),)

def pinject(prop, func, **kw):
    """Transform a list function into a table function.

    'func' must be a function taking a list.  This will return a
    function that takes a table and applies 'func' to the projection
    of the property 'prop'.  The returned function will be given the
    same name as 'func' (which makes it handy as an argument to
    group_by).  Keyword arguments will be forwarded to
    AbstractTable.prop."""

    f = lambda table: func(table.prop(prop, **kw))
    f.__name__ = func.__name__
    return f
