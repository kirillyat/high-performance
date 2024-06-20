import org.junit.Test;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Iterator;

import static org.junit.Assert.*;

public class SetTest {
    @Test
    public void isEmpty() {
        SetImpl<Integer> set = new SetImpl<>();
        assertTrue(set.isEmpty());
        set.add(1);
        assertFalse(set.isEmpty());
        set.remove(1);
        assertTrue(set.isEmpty());
    }

    @Test
    public void add() {
        SetImpl<Integer> set = new SetImpl<>();
        assertTrue(set.add(1));
        assertFalse(set.add(1));
        assertTrue(set.add(2));
        assertFalse(set.add(1));
        assertFalse(set.add(2));
        assertFalse(set.add(1));
        set.remove(1);
        assertTrue(set.add(1));
    }

    @Test
    public void remove() {
        SetImpl<Integer> set = new SetImpl<>();
        set.add(1);
        assertTrue(set.remove(1));
        assertFalse(set.remove(1));
    }

    @Test
    public void addContains() {
        SetImpl<Integer> set = new SetImpl<>();
        assertFalse(set.contains(1));
        set.add(1);
        assertTrue(set.contains(1));
        set.add(2);
        assertTrue(set.contains(2));
        assertFalse(set.contains(0));
        set.remove(2);
        assertFalse(set.contains(2));
    }

    @Test
    public void iterator() {
        SetImpl<Integer> set = new SetImpl<>();
        set.add(1);
        set.add(2);
        set.add(3);
        set.remove(3);
        Iterator<Integer> it = set.iterator();
        ArrayList<Integer> elems = new ArrayList<>();
        it.forEachRemaining(elems::add);
        Collections.sort(elems);
        assertEquals(Arrays.asList(1, 2), elems);
    }

}