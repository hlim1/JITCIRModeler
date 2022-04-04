function sign(x) {
    return Math.sign(x);
}

let result;
for (let i = 0; i < 10000; i++) {
    result = sign(3);
    result = sign(-3);
}
