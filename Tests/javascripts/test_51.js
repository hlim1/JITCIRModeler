// Reference: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/parseInt

function roughScale(x, base) {
      const parsed = parseInt(x, base);
      if (isNaN(parsed)) { return 0; }
      return parsed * 100;
}

let result;
for (let i = 0; i < 10000; i++) {
    result = roughScale(' 0xF', 16);
}
