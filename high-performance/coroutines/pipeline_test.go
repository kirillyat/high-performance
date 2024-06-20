package pipeline

import (
	"crypto/md5"
	"fmt"
	"hash/crc32"
	"strconv"
	"sync/atomic"
	"testing"
	"time"
)

func TestPipelineFlow(t *testing.T) {
	var isValid = true
	var count uint32
	pipelineJobs := []job{
		job(func(in, out chan interface{}) {
			out <- 1
			time.Sleep(10 * time.Millisecond)
			if atomic.LoadUint32(&count) == 0 {
				isValid = false
			}
		}),
		job(func(in, out chan interface{}) {
			for range in {
				atomic.AddUint32(&count, 1)
			}
		}),
	}
	PipelineExecutor(pipelineJobs...)
	if !isValid || count == 0 {
		t.Errorf("Pipeline flow failed - data not transferred correctly")
	}
}

func TestHashing(t *testing.T) {
	expectedResult := "1173136728138862632818075107442090076184424490584241521304_1696913515191343735512658979631549563179965036907783101867_27225454331033649287118297354036464389062965355426795162684_29568666068035183841425683795340791879727309630931025356555_3994492081516972096677631278379039212655368881548151736_4958044192186797981418233587017209679042592862002427381542_4958044192186797981418233587017209679042592862002427381542"
	var result string

	// Redefine the hashing functions to include counters and locks
	var (
		DataSignerSalt = ""
		lockCounter    uint32
		unlockCounter  uint32
		md5Counter     uint32
		crc32Counter   uint32
	)

	OverheatLock = func() {
		atomic.AddUint32(&lockCounter, 1)
		for {
			if atomic.CompareAndSwapUint32(&dataSignerOverheat, 0, 1) {
				break
			}
			time.Sleep(time.Second)
		}
	}
	OverheatUnlock = func() {
		atomic.AddUint32(&unlockCounter, 1)
		for {
			if atomic.CompareAndSwapUint32(&dataSignerOverheat, 1, 0) {
				break
			}
			time.Sleep(time.Second)
		}
	}
	DataSignerMd5 = func(data string) string {
		atomic.AddUint32(&md5Counter, 1)
		OverheatLock()
		defer OverheatUnlock()
		hash := fmt.Sprintf("%x", md5.Sum([]byte(data+DataSignerSalt)))
		time.Sleep(10 * time.Millisecond)
		return hash
	}
	DataSignerCrc32 = func(data string) string {
		atomic.AddUint32(&crc32Counter, 1)
		hash := crc32.ChecksumIEEE([]byte(data + DataSignerSalt))
		time.Sleep(time.Second)
		return strconv.FormatUint(uint64(hash), 10)
	}

	input := []int{0, 1, 1, 2, 3, 5, 8}
	jobs := []job{
		job(func(in, out chan interface{}) {
			for _, num := range input {
				out <- num
			}
		}),
		job(SingleHash),
		job(MultiHash),
		job(CombineResults),
		job(func(in, out chan interface{}) {
			raw := <-in
			data, ok := raw.(string)
			if !ok {
				t.Error("Cannot convert result to string")
			}
			result = data
		}),
	}

	startTime := time.Now()
	PipelineExecutor(jobs...)
	duration := time.Since(startTime)

	maxDuration := 4 * time.Second

	if expectedResult != result {
		t.Errorf("Results do not match\nExpected: %v\nGot: %v", expectedResult, result)
	}

	if duration >= maxDuration {
		t.Errorf("Execution took too long\nExpected: <%v\nGot: %v", maxDuration, duration)
	}

	expectedMd5Calls := len(input)
	expectedCrc32Calls := len(input) * 8
	if int(lockCounter) != expectedMd5Calls ||
		int(unlockCounter) != expectedMd5Calls ||
		int(md5Counter) != expectedMd5Calls ||
		int(crc32Counter) != expectedCrc32Calls {
		t.Errorf("Incorrect number of hash function calls\nMd5: %v\nCrc32: %v", md5Counter, crc32Counter)
	}
}

func TestOverheatUnlock(t *testing.T) {
	dataSignerOverheat = 1

	done := make(chan struct{})
	go func() {
		OverheatUnlock()
		done <- struct{}{}
	}()

	select {
	case <-done:
		// Test passed
	case <-time.After(3 * time.Second):
		t.Errorf("OverheatUnlock did not release the lock in time")
	}
}

func TestDataSignerMd5(t *testing.T) {
	data := "testdata"
	expectedMd5 := fmt.Sprintf("%x", md5.Sum([]byte(data+DataSignerSalt)))

	var lockCounter, unlockCounter uint32
	OverheatLock = func() {
		atomic.AddUint32(&lockCounter, 1)
		for {
			if atomic.CompareAndSwapUint32(&dataSignerOverheat, 0, 1) {
				break
			}
		}
	}
	OverheatUnlock = func() {
		atomic.AddUint32(&unlockCounter, 1)
		for {
			if atomic.CompareAndSwapUint32(&dataSignerOverheat, 1, 0) {
				break
			}
		}
	}

	md5Result := DataSignerMd5(data)

	if md5Result != expectedMd5 {
		t.Errorf("MD5 hash incorrect\nExpected: %v\nGot: %v", expectedMd5, md5Result)
	}

	if lockCounter != 1 {
		t.Errorf("OverheatLock should have been called once\nGot: %v", lockCounter)
	}

	if unlockCounter != 1 {
		t.Errorf("OverheatUnlock should have been called once\nGot: %v", unlockCounter)
	}
}

func TestDataSignerCrc32(t *testing.T) {
	data := "testdata"
	expectedCrc32 := crc32.ChecksumIEEE([]byte(data + DataSignerSalt))
	expectedCrc32Str := strconv.FormatUint(uint64(expectedCrc32), 10)

	crc32Result := DataSignerCrc32(data)

	if crc32Result != expectedCrc32Str {
		t.Errorf("CRC32 hash incorrect\nExpected: %v\nGot: %v", expectedCrc32Str, crc32Result)
	}
}
