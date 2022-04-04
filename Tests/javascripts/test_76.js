function log1p(x) {
    return Math.log1p(x);
}

let result;
for (let i = 0; i < 10000; i++) {
    result = log1p(2);
}
