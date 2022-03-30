// Reference: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Bigint64Array

function bigint64array() {
    let iterable = function*(){ yield* [1n, 2n, 3n]; }();
    let arr2 = new BigInt64Array(iterable);
}

for (let i = 0; i < 10000; i++) {
    bigint64array();
}
