function max(x, y, z) {
    return Math.max(x, y, z);
}

let result;
for (let i = 0; i < 10000; i++) {
    result = max(2, 5, 3);
}
