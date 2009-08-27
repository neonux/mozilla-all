// |trace-test| TMFLAGS: full

function arith()
{
  var accum = 0;
  for (var i = 0; i < 100; i++) {
    accum += (i * 2) - 1;
  }
  return accum;
}
assertEq(arith(), 9800);
