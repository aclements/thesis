def commutes2(s0, opA, argsA, opB, argsB):
  # Run reordering [opA, opB] starting from s0
  sAB = s0.copy()
  rA  = opA(sAB, *argsA)
  rAB = opB(sAB, *argsB)

  # Run reordering [opB, opA] starting from s0
  sBA = s0.copy()
  rB  = opB(sBA, *argsB)
  rBA = opA(sBA, *argsA)

  # Test commutativity
  return rA == rBA and rB == rAB and sAB == sBA
