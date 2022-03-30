// Reference: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/BigUint64Array

function biguint64array() {
    let iterable = function*(){ yield* [1n, 2n, 3n]; }();
    let arr2 = new BigUint64Array(iterable);
}

for (let i = 0; i < 10000; i++) {
    biguint64array();
}
