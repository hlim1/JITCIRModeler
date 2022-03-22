function min(x, y, z) {
    return Math.min(x, y, z);
}

let result;
for (let i = 0; i < 10000; i++) {
    result = min(2, 5, 3);
}
