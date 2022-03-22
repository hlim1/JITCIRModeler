function pow(x, y) {
    return Math.pow(x, y);
}

let result;
for (let i = 0; i < 10000; i++) {
    result = pow(2, 5);
}
