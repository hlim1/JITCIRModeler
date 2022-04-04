function trunc(x) {
    return Math.trunc(x);
}

let result;
for (let i = 0; i < 10000; i++) {
    result = trunc(13.14);
}
