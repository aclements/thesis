def commutes(s0, ops, args):
  states = {frozenset(): s0}
  results = {}

  # Generate all (non-empty) prefixes of all reorderings of ops
  todo = list((op,) for op in ops)
  while todo:
    perm = todo.pop()

    # Execute next operation in this permutation
    past, op = perm[:-1], perm[-1]
    s = states[frozenset(past)].copy()
    r = op(s, args[op])

    # Test for result equivalence
    if op not in results:
      results[op] = r
    elif r != results[op]:
      return False

    # Test for state equivalence
    if frozenset(perm) not in states:
      states[frozenset(perm)] = s
    elif s != states[frozenset(perm)]:
      return False

    # Queue all extensions of perm
    todo.extend(perm + (nextop,) for nextop in ops if nextop not in perm)
  return True
