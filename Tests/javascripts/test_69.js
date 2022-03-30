// Reference: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Int16Array

function int16array(x) {
    let int16 = new Int16Array(x);
    int16[0] = 41;
}

for (let i = 0; i < 10000; i++) {
    int16array([21,31]);
}
