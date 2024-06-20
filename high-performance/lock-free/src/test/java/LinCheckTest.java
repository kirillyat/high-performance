import java.util.ArrayList;
import java.util.List;

import org.jetbrains.kotlinx.lincheck.LinChecker;
import org.jetbrains.kotlinx.lincheck.annotations.Operation;
import org.jetbrains.kotlinx.lincheck.annotations.Param;
import org.jetbrains.kotlinx.lincheck.paramgen.IntGen;
import org.jetbrains.kotlinx.lincheck.strategy.stress.StressCTest;

import org.junit.Test;

@Param(name = "value", gen = IntGen.class)
@StressCTest(requireStateEquivalenceImplCheck = false)
public class LinCheckTest {
    private final SetImpl<Integer> set = new SetImpl<>();

    @Operation
    public boolean add(@Param(name = "value") final Integer value) {
        return set.add(value);
    }

    @Operation
    public boolean remove(@Param(name = "value") final Integer value) {
        return set.remove(value);
    }

    @Operation
    public boolean contains(@Param(name = "value") final Integer value) {
        return set.contains(value);
    }

    @Operation
    public boolean isEmpty() {
        return set.isEmpty();
    }

    @Operation
    public List<Integer> iterator() {
        final var list = new ArrayList<Integer>();
        set.iterator().forEachRemaining(list::add);
        return list;
    }

    @Test
    public void runTest() {
        LinChecker.check(LinCheckTest.class);
    }
}