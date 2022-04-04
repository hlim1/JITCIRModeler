function expm1(x) {
    return Math.expm1(x);
}

let result;
for (let i = 0; i < 10000; i++) {
    result = expm1(3.14);
}
