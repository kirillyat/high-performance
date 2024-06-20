import java.util.concurrent.atomic.AtomicMarkableReference;

class Node<T> {
    final T value;
    final AtomicMarkableReference<Node<T>> next;

    Node(final T value, final AtomicMarkableReference<Node<T>> next) {
        this.value = value;
        this.next = next;
    }
}
