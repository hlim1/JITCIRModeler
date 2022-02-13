function absolute(x) {
    return Math.abs(x);
}

let result;
for (let i = 0; i < 10000; i++) {
    result = absolute(-3.14);
}
