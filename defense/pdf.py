import decimal, re

DELIM = '][\0\t\n\f\r ()<>{}/%'

class File(object):
    def __init__(self, fname):
        self.__data = open(fname, 'rb').read()
        self.__updates = {}

        # Parse the trailer (XXX should check for line endings)
        self.seek(0)
        if not self.__data.startswith('%PDF-1.'):
            self.__syntax_error('Not a PDF file (missing header)')
        pos = self.__data.rfind('%%EOF')
        if pos < 0:
            self.__syntax_error('Missing EOF marker')
        # XXX The trailer could actually start further back
        self.seek(self.__data.rfind('trailer', 0, pos))
        self.read_keyword('trailer')
        self.trailer = self.read()
        self.read_keyword('startxref')
        self.__xrefpos = self.read()
        if not isinstance(self.__xrefpos, int):
            self.__syntax_error('Expected xref position')
        self.__next_objid = self.trailer['Size']

    def seek(self, offset):
        """Seek to a byte offset."""

        self.__pos = offset

    def read(self):
        """Read the PDF object at the current position."""

        # Skip white space
        self.__ws()

        data = self.__data
        ch = data[self.__pos]
        # Boolean [PDF1.5 3.2.1]
        if ch == 't':
            self.read_keyword('true')
            return True
        if ch == 'f':
            self.read_keyword('false')
            return False
        # Numeric [PDF1.5 3.2.2]
        if ch in '0123456789-.':
            res = self.__read_number()
            lapos = self.__pos
            self.__ws()
            if isinstance(res, int) and self.__pos < len(self.__data) and \
               self.__data[self.__pos] in '0123456789':
                gen = self.__read_number()
                self.__ws()
                if self.read_keyword() == 'R':
                    # Actually it's an indirect [PDF1.5 3.2.9]
                    return R(res, gen, self)
            self.__pos = lapos
            return res
        # String [PDF1.5 3.2.3]
        if ch == '(':
            return self.__read_literal_string()
        if ch == '<' and data[self.__pos + 1] != '<':
            return self.__read_hex_string()
        # Name [PDF1.5 3.2.4]
        if ch == '/':
            return self.__read_name()
        # Array [PDF1.5 3.2.5]
        if ch == '[':
            return self.__read_array()
        # Dictionary [PDF1.5 3.2.6]
        if ch == '<' and self.__data[self.__pos + 1] == '<':
            d = self.__read_dictionary()
            lapos = self.__pos
            if self.read_keyword() != 'stream':
                self.__pos = lapos
                return d
            # Actually it's a stream [PDF1.5 3.2.7]
            raise NotImplementedError('Stream parsing')
        # Null [PDF1.5 3.2.8]
        if ch == 'n':
            self.read_keyword('null')
            return None
        self.__syntax_error('Unknown syntax')

    def read_keyword(self, require=None):
        self.__ws()
        start = pos = self.__pos
        # Read until whitespace or delimiter [PDF1.5 3.1.1]
        while self.__data[pos] not in DELIM:
            pos += 1
        res = self.__data[start:pos]
        if require is not None and res != require:
            self.__syntax_error('Expected keyword %r, got %r' % (require, res))
        self.__pos = pos
        return res

    def __ws(self):
        data, pos, end = self.__data, self.__pos, len(self.__data)
        while pos < end and data[pos] in '\0\t\n\f\r %':
            if data[pos] == '%':
                while data[pos] not in '\r\n':
                    pos += 1
            pos += 1
        self.__pos = pos

    def __read_number(self):
        tok = self.read_keyword()
        if '.' in tok:
            return float(tok)
        return int(tok)

    def __read_literal_string(self):
        ESC = {'n':'\n', 'r':'\r', 't':'\t', 'b':'\b', 'f':'\f'}
        data, pos, depth = self.__data, self.__pos + 1, 0
        res = []
        while True:
            npos = data.find(('\\', ')'))
            if npos == -1:
                self.__syntax_error('Unterminated string')
            res.append(data[pos:npos])
            pos = npos
            if data[pos] == ')':
                depth -= 1
                if depth < 0:
                    break
                res.append(data[pos])
                pos += 1
            elif data[pos] == '\\':
                pos += 1
                if data[pos] in '01234567':
                    res.append(int(data[pos:pos+3], 8))
                    pos += 3
                elif data[pos] in ESC:
                    res.append(ESC[data[pos]])
                    pos += 1
                elif data[pos] == '\n':
                    pos += 1
                elif data[pos] == '\r':
                    pos += 1
                    if data[pos] == '\n':
                        pos += 1
                # Otherwise ignore the backslash
        self.__pos = pos + 1
        return ''.join(pos)

    def __read_hex_string(self):
        start = self.__pos + 1
        end = self.__data.find('>', start)
        if end < 0:
            self.__syntax_error('Unterminated hex string')
        s = self.__data[start:end].translate(None, '\0\t\n\f\r ')
        if len(s) % 2 == 1:
            s += '0'
        res = ''.join(chr(int(s[i:i+2], 16)) for i in range(0, len(s), 2))
        self.__pos = end + 1
        return HexStr(res)

    def __read_name(self):
        self.__pos += 1
        tok = self.read_keyword()
        if '#' in tok:
            tok = re.sub('#(..)', lambda m: chr(int(m.group(1), 16)), tok)
        return Name(tok)

    def __read_array(self):
        self.__pos += 1
        res = []
        while self.__data[self.__pos] != ']':
            res.append(self.read())
            self.__ws()
        self.__pos += 1
        return res

    def __read_dictionary(self):
        self.__pos += 2
        res = {}
        while not self.__data.startswith('>>', self.__pos):
            k = self.read()
            if not isinstance(k, Name):
                self.__syntax_error('Dictionary key is not a name')
            res[k.name] = self.read()
            self.__ws()
        self.__pos += 2
        return res

    def __object_offset(self, objid, gen):
        self.seek(self.__xrefpos)
        while True:
            self.read_keyword('xref')
            self.__ws()
            while True:
                if self.__data.startswith('trailer', self.__pos):
                    # Done with this xref table
                    break
                first, n = self.__read_number(), self.__read_number()
                self.__ws()
                if first <= objid < first + n:
                    pos = self.__pos + (objid - first) * 20
                    try:
                        offset, xgen, typ = int(self.__data[pos:pos+10]), \
                                            int(self.__data[pos+11:pos+16]), \
                                            self.__data[pos+17]
                        if typ not in 'nf':
                            raise ValueError()
                    except ValueError:
                        self.seek(pos)
                        self.__syntax_error('Bad xref entry')
                    if typ == 'n' or gen == xgen:
                        # Found it
                        return offset
                # Seek to the next section in this xref table
                self.seek(self.__pos + n * 20)

            # Seek to the previous xref table
            self.read_keyword('trailer')
            trailer = self.read()
            if 'Prev' not in trailer:
                # No more xref tables
                return None
            self.seek(trailer['Prev'])

    def _get_object(self, objid, gen):
        # Get the object's offset
        offset = self.__object_offset(objid, gen)
        if offset is None:
            # A bad reference is equivalent to the null object
            return None
        self.seek(offset)

        # Read the object
        xobjid, xgen = self.__read_number(), self.__read_number()
        if xobjid != objid or xgen != gen:
            self.__syntax_error('xref table points to wrong object')
        self.read_keyword('obj')
        res = self.read()
        self.read_keyword('endobj')
        return res

    def add_obj(self, val):
        # Just pick the next highest object ID, rather than trying to
        # reuse one
        objid = self.__next_objid
        self.__next_objid += 1
        pos = len(self.__data)
        self.__updates[objid] = (pos, 0)
        self.__data += '%d %d obj\n%s\nendobj\n' % (objid, 0, show(val))
        return R(objid, 0, self)

    def _replace_obj(self, objid, gen, val):
        pos = len(self.__data)
        self.__updates[objid] = (pos, gen)
        self.__data += '%d %d obj\n%s\nendobj\n' % (objid, gen, show(val))

    def to_str(self):
        # Flush updates
        if self.__updates:
            xrefpos = len(self.__data)
            self.__data += 'xref\n'
            for objid, (off, gen) in sorted(self.__updates.items()):
                self.__data += '%d %d\n%010d %05d n \n' % (objid, 1, off, gen)
            self.trailer['Prev'] = self.__xrefpos
            self.__xrefpos = xrefpos
            self.__data += 'trailer\n%s\nstartxref\n%d\n%%EOF\n' % \
                           (show(self.trailer), xrefpos)
            self.__updates = {}
        return self.__data

    def write_to(self, fname):
        open(fname, 'wb').write(self.to_str())

class HexStr(str):
    """A PDF hex string."""
    pass

class Name(object):
    """A PDF name object."""
    def __init__(self, name):
        self.name = name

    def __eq__(self, o):
        if not isinstance(o, Name):
            return NotImplemented
        return self.name == o.name

    def __hash__(self):
        return hash(self.name)

    def __repr__(self):
        return 'Name(' + repr(self.name) + ')'

class R(object):
    """An indirect reference to another object."""
    def __init__(self, objid, gen, file):
        self.objid, self.gen, self.file = objid, gen, file

    def __repr__(self):
        return 'R(%d,%d)' % (self.objid, self.gen)

    def replace(self, val):
        self.file._replace_obj(self.objid, self.gen, val)

def deref(obj):
    """Dereference a PDF object."""
    if not isinstance(obj, R):
        return obj
    return obj.file._get_object(obj.objid, obj.gen)

def show(val):
    """Convert val to PDF object syntax."""
    if val is True:
        return 'true'
    if val is False:
        return 'false'
    if isinstance(val, int):
        return str(val)
    if isinstance(val, float):
        d = decimal.Decimal(val)
        d = d.quantize(decimal.Decimal(1)) if d == d.to_integral() \
            else d.normalize()
        return str(d)
    if isinstance(val, HexStr):
        return '<' + ''.join('%02x' % ord(c) for c in val) + '>'
    if isinstance(val, str):
        ESC = {'\n':'\\n', '\r':'\\r', '\t':'\\t', '\b':'\\b', '\f':'\\f',
               '(':'\\(', ')':'\\)', '\\':'\\\\'}
        return '(' + re.sub('[\n\r\t\b\f()\\]', val,
                            lambda m:ESC[m.group(0)]) + ')'
    if isinstance(val, Name):
        return '/' + re.sub('[' + DELIM + ']',
                            lambda m: '#%02x' % ord(m.group(0)), val.name)
    if isinstance(val, list):
        return '[' + ' '.join(map(show, val)) + ']'
    if isinstance(val, dict):
        return '<<\n' + ''.join('%s %s\n' % (show(Name(k)), show(v))
                                for k, v in val.iteritems()) + '>>'
    if val is None:
        return 'null'
    if isinstance(val, R):
        return '%d %d R' % (val.objid, val.gen)
    raise ValueError('Not a valid PDF object: %r' % val)
