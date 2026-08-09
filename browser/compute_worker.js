self.onmessage = function(e) {
    const task = e.data;
    let result = null;
    try {
        // Simple compute: pattern evaluation
        if (task.type === "PATTERN_EVALUATION") {
            result = task.payload.reduce((a, b) => a + b, 0) / task.payload.length;
        }
    } catch(err) {
        result = null;
    }
    self.postMessage({ taskId: task.taskId, result });
};
