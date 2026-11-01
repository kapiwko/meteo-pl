.pragma library
// A request for the displayed frame always wins over an in-flight future frame.
function action(requested, displayed, pending) {
    if (requested === displayed) return "displayed";
    if (requested === pending) return "pending";
    return "replace";
}
function current(candidate, pending) { return candidate !== null && candidate === pending; }
