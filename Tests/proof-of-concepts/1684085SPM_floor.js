with ({}) {}

function floor(a, b) {
        return Math.floor(a / b) | 0;
}

for (var i = 0; i < 50; i++) {
        floor(5,5);
}

// Expected Output: -2
print(floor(-5,3));
