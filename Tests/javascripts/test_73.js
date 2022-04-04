function cbrt(x) {
    return Math.cbrt(x);
}

let result;
for (let i = 0; i < 10000; i++) {
    result = cbrt(3.14);
}
