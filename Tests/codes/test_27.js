function arcsine(x) {
    return Math.asin(x);
}

let result;
for (let i = 0; i < 10000; i++) {
    result = arcsine(0.5);
}
