class BubbleDB {
    constructor(name = "wave-bubble") {
        this.name = name;
        this.db = null;
    }

    async open() {
        this.db = await new Promise((resolve, reject) => {
            const request = indexedDB.open(this.name, 1);
            request.onupgradeneeded = () => {
                const db = request.result;
                if (!db.objectStoreNames.contains("traces")) {
                    const store = db.createObjectStore("traces", { keyPath: "key" });
                    store.createIndex("rhythm", "rhythm");
                    store.createIndex("phase", "phase");
                    store.createIndex("timestamp", "timestamp");
                }
            };
            request.onsuccess = () => resolve(request.result);
            request.onerror = () => reject(request.error);
        });
        return this;
    }

    async put(trace) {
        return new Promise((resolve, reject) => {
            const tx = this.db.transaction("traces", "readwrite");
            const store = tx.objectStore("traces");
            store.put(trace);
            tx.oncomplete = () => resolve(trace);
            tx.onerror = () => reject(tx.error);
        });
    }

    async get(key) {
        return new Promise((resolve, reject) => {
            const tx = this.db.transaction("traces", "readonly");
            const request = tx.objectStore("traces").get(key);
            request.onsuccess = () => resolve(request.result || null);
            request.onerror = () => reject(request.error);
        });
    }

    async remove(key) {
        return new Promise((resolve, reject) => {
            const tx = this.db.transaction("traces", "readwrite");
            tx.objectStore("traces").delete(key);
            tx.oncomplete = () => resolve();
            tx.onerror = () => reject(tx.error);
        });
    }
}
