package pipeline

import (
	"crypto/md5"
	"fmt"
	"hash/crc32"
	"sort"
	"strconv"
	"strings"
	"sync"
	"sync/atomic"
	"time"
)

type job func(in, out chan interface{})

var (
	dataSignerOverheat uint32 = 0
	DataSignerSalt            = ""
)

var OverheatLock = func() {
	for {
		if atomic.CompareAndSwapUint32(&dataSignerOverheat, 0, 1) {
			break
		}
		fmt.Println("OverheatLock happened")
		time.Sleep(time.Second)
	}
}

var OverheatUnlock = func() {
	for {
		if atomic.CompareAndSwapUint32(&dataSignerOverheat, 1, 0) {
			break
		}
		fmt.Println("OverheatUnlock happened")
		time.Sleep(time.Second)
	}
}

var DataSignerMd5 = func(data string) string {
	OverheatLock()
	defer OverheatUnlock()

	data += DataSignerSalt
	hash := fmt.Sprintf("%x", md5.Sum([]byte(data)))
	time.Sleep(10 * time.Millisecond)
	return hash
}

var DataSignerCrc32 = func(data string) string {
	crcH := crc32.ChecksumIEEE([]byte(data + DataSignerSalt))
	time.Sleep(time.Second)
	return strconv.FormatUint(uint64(crcH), 10)
}

func PipelineExecutor(jobs ...job) {
	var wg sync.WaitGroup
	in := make(chan interface{})

	for _, j := range jobs {
		out := make(chan interface{})
		wg.Add(1)
		go func(j job, in, out chan interface{}) {
			defer wg.Done()
			j(in, out)
			close(out)
		}(j, in, out)
		in = out
	}

	wg.Wait()
}

func SingleHash(in, out chan interface{}) {
	var wg sync.WaitGroup
	var mu sync.Mutex

	for i := range in {
		wg.Add(1)
		go func(data int) {
			defer wg.Done()

			strData := strconv.Itoa(data)
			mu.Lock()
			md5Hash := DataSignerMd5(strData)
			mu.Unlock()

			crc32Chan := make(chan string)
			go func(data string) {
				crc32Chan <- DataSignerCrc32(data)
			}(strData)

			crc32Str := <-crc32Chan
			crc32Md5Str := DataSignerCrc32(md5Hash)

			out <- crc32Str + "~" + crc32Md5Str
		}(i.(int))
	}

	wg.Wait()
}

func MultiHash(in, out chan interface{}) {
	var wg sync.WaitGroup

	for data := range in {
		wg.Add(1)
		go func(data string) {
			defer wg.Done()
			var mu sync.Mutex
			var internalWg sync.WaitGroup
			hashes := make([]string, 6)

			for i := 0; i < 6; i++ {
				internalWg.Add(1)
				go func(i int, data string) {
					defer internalWg.Done()
					hash := DataSignerCrc32(strconv.Itoa(i) + data)
					mu.Lock()
					hashes[i] = hash
					mu.Unlock()
				}(i, data)
			}

			internalWg.Wait()
			out <- strings.Join(hashes, "")
		}(data.(string))
	}

	wg.Wait()
}

func CombineResults(in, out chan interface{}) {
	var results []string

	for data := range in {
		results = append(results, data.(string))
	}

	sort.Strings(results)
	out <- strings.Join(results, "_")
}
