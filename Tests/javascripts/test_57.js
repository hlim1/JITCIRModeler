// Reference: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/BigInt

let result;

function bigInt(x) {
    return BigInt(x);
}

for (let i = 0; i < 10000; i++) {
    result = bigInt("0b11111111111111111111111111111111111111111111111111111");
}