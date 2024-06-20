import java.util.AbstractMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.ArrayList;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.stream.Collectors;

public class SetImpl<T extends Comparable<T>> implements Set<T> {
    private final Node<T> head = new Node<>(null, new AtomicMarkableReference<>(null, false));

    @Override
    public boolean add(final T element) {
        Node<T> newNode = new Node<>(element, new AtomicMarkableReference<>(null, false));

        while (true) {
            Map.Entry<Node<T>, Node<T>> existingNode = find(element);
            Node<T> previousNode = existingNode.getKey();
            Node<T> existingElement = existingNode.getValue();

            if (existingElement != null) {
                return false;
            }

            if (previousNode.next.compareAndSet(null, newNode, false, false)) {
                return true;
            }
        }
    }

    @Override
    public boolean remove(final T element) {
        while (true) {
            Map.Entry<Node<T>, Node<T>> entry = find(element);
            Node<T> target = entry.getValue();
            if (target == null) {
                return false;
            }
            Node<T> successor = target.next.getReference();
            if (target.next.compareAndSet(successor, successor, false, true)) {
                return true;
            }
        }
    }

    @Override
    public boolean contains(final T element) {
        Node<T> currentNode = head.next.getReference();
        while (currentNode != null) {
            if (currentNode.value.equals(element) && !currentNode.next.isMarked()) {
                return true;
            }
            currentNode = currentNode.next.getReference();
        }
        return false;
    }

    @Override
    public boolean isEmpty() {
        return getSnapshot().isEmpty();
    }

    @Override
    public Iterator<T> iterator() {
        return getSnapshot().iterator();
    }

    private Map.Entry<Node<T>, Node<T>> find(final T value) {
        while (true) {
            boolean restart = false;
            Node<T> currentNode = head;
            Node<T> nextNode = head.next.getReference();
            while (nextNode != null && !restart) {
                final AtomicMarkableReference<Node<T>> nextReference = nextNode.next;
                if (nextReference.isMarked()) {
                    currentNode.next.compareAndSet(nextNode, nextReference.getReference(), false, false);
                    restart = true;
                } else {
                    if (nextNode.value.equals(value)) {
                        return new AbstractMap.SimpleEntry<>(currentNode, nextNode);
                    }
                    currentNode = nextNode;
                    nextNode = nextReference.getReference();
                }
            }
            if (!restart) {
                return new AbstractMap.SimpleEntry<>(currentNode, null);
            }
        }
    }

    private List<T> getSnapshot() {
        List<Node<T>> firstSnapshot = collect();
        while (true) {
            List<Node<T>> secondSnapshot = collect();
            if (firstSnapshot.equals(secondSnapshot)) {
                return firstSnapshot.stream()
                        .map(node -> node.value)
                        .collect(Collectors.toList());
            }
            firstSnapshot = secondSnapshot;
        }
    }

    private List<Node<T>> collect() {
        final List<Node<T>> list = new ArrayList<>();
        Node<T> currentNode = head.next.getReference();
        while (currentNode != null) {
            if (!currentNode.next.isMarked()) {
                list.add(currentNode);
            }
            currentNode = currentNode.next.getReference();
        }
        return list;
    }
}