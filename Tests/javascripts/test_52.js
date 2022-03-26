// Reference: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/parseFloat

function circumference(r) {
      return parseFloat(r) * 2.0 * Math.PI;
}

let result;
for (let i = 0; i < 10000; i++) {
    result = circumference(4.567);
}
