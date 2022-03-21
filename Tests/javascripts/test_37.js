function round(x) {
    return Math.round(x);
}

let result;
for (let i = 0; i < 10000; i++) {
    result = round(1.234);
}
