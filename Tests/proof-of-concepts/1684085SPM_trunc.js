with ({}) {}

function trunc(a, b) {
        return Math.trunc(a / b) | 0;
}

for (var i = 0; i < 50; i++) {
        trunc(5,5);
}

// Expected Output: 1
print(trunc(5,3));
