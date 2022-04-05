// Reference: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Int16Array

function int16array() {
    let iterable = function*(){ yield* [1, 2, 3]; }();
    let arr2 = new Int16Array(iterable);
}

for (let i = 0; i < 10000; i++) {
    int16array();
}
