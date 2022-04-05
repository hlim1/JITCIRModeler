function isnan(x) {
    return isNaN(x);
}

let result;
for (let i = 0; i < 10000; i++) {
    result = isnan("This is not a number");
    result = isnan("100");
}
