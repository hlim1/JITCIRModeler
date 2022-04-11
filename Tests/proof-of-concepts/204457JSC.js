function testMax(x, y) {
    return Math.max(x, y);
}
noInline(testMax);

for (var i = 0; i < 500; ++i) {
    x = 100/testMax(0.0, -0.0);
}
