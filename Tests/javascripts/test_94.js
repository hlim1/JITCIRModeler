// Reference: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/UInt16Array

function uint16array() {
    let iterable = function*(){ yield* [1, 2, 3]; }();
    let arr2 = new Uint16Array(iterable);
}

for (let i = 0; i < 10000; i++) {
    uint16array();
}
