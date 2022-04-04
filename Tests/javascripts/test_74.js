function hypot(x, y, z) {
    var r;
    r = Math.hypot(x);
    r = Math.hypot(x, y);
    r = Math.hypot(x, y, z);

    return r;
}

let result;
for (let i = 0; i < 10000; i++) {
    result = hypot(3, 4, 5);
}
