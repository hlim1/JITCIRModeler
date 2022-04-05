// Reference: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Int32Array

function int32array() {
    let iterable = function*(){ yield* [1, 2, 3]; }();
    let arr2 = new Int32Array(iterable);
}

for (let i = 0; i < 10000; i++) {
    int32array();
}
