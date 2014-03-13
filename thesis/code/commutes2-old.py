def commutes(state_type, opA, opB):
  # Create unconstrained symbolic initial state and arguments
  s0    = state_type.any()
  argsA = opA.args_type.any()
  argsB = opB.args_type.any()

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
