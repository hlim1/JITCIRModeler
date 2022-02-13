function arctan(x) {
    return Math.atan(x);
}

let result;
for (let i = 0; i < 10000; i++) {
    result = arctan(0.5);
}
