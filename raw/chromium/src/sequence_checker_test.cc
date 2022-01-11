namespace {
    X HeavyJob(int a, base::OnceCallback<void()> callback) {
        X x = findX(a);
        std::move(callback).Run();
        return x;
    }
}

void Object::DoHeavyJob(int a) {
    base::PostTaskAndReplyWithResult(
        background_task_runner_.get(), FROM_HERE,
        base::BindOnce(&HeavyJob, a,
            base::BindOnce(&Object::AdditionalCallback, AsWeakPtr())),
        base::BindOnce(&Object::OnHeavyJobFinish, AsWeakPtr())
    );
}

void Object::OnHeavyJobFinish(const X& x) {
    // ...
}

void Object::AdditionalCallback() {
}