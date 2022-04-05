// Reference: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/UInt8Array

function uint8array() {
    let iterable = function*(){ yield* [1, 2, 3]; }();
    let arr2 = new Uint8Array(iterable);
}

for (let i = 0; i < 10000; i++) {
    uint8array();
}
