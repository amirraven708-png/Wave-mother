class ComputeScheduler {
    constructor() {
        this.workers = [];
        this.taskQueue = [];
        this.maxWorkers = navigator.hardwareConcurrency || 4;
    }

    async init() {
        for (let i = 0; i < this.maxWorkers; i++) {
            const worker = new Worker("compute_worker.js");
            worker.onmessage = (e) => this.onTaskComplete(e.data);
            this.workers.push(worker);
        }
    }

    submit(task) {
        this.taskQueue.push(task);
        this.dispatch();
    }

    dispatch() {
        const idle = this.workers.find(w => !w.busy);
        if (!idle || this.taskQueue.length === 0) return;
        const task = this.taskQueue.shift();
        idle.busy = true;
        idle.postMessage(task);
    }

    onTaskComplete(result) {
        const worker = this.workers.find(w => w.busy);
        if (worker) worker.busy = false;
        if (result.callback) result.callback(result);
        this.dispatch();
    }
}
