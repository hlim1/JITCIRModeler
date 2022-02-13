function harccosine(x) {
    return Math.acosh(x);
}

let result;
for (let i = 0; i < 10000; i++) {
    result = harccosine(-3.14);
}
