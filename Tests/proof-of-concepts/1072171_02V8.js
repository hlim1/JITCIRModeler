function crash() {
    let confused;
    for (let i = -0.0; i < 1000; i++) {
        confused = Math.max(-1, i);
    }
    confused[0];
}

crash();
%OptimizeFunctionOnNextCall(crash);
crash();
