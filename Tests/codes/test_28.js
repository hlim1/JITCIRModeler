function harcsine(x) {
    return Math.asinh(x);
}

let result;
for (let i = 0; i < 10000; i++) {
    result = harcsine(1);
}
