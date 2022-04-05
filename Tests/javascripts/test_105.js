function isfinite(x, y) {
    return isFinite(x / y);
}

let result;
for (let i = 0; i < 10000; i++) {
    result = isfinite(1000, 0);
}
