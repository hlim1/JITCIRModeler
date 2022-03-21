function divide(x, y) {
    return x / y;
}

let result_int;
let result_float;
for (let i = 0; i < 10000; i++) {
    result_int = divide(2, 4);
    result_float = divide(1, 2);
}
