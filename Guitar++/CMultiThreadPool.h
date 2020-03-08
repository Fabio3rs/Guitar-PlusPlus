#pragma once
#ifndef _CMultiThreadPool_H_
#define _CMultiThreadPool_H_

#include <bitset>
#include <memory>
#include <cstdint>
#include <vector>
#include <utility>
#include <mutex>
#include <atomic>
#include <thread>

template <class poolData_t, const size_t numElements>
class CMultiThreadPool
{
	std::bitset<numElements> bits;
	std::unique_ptr<poolData_t[]> pool;
	std::atomic<size_t> firstFreeElement;
	std::atomic<size_t> addedElements;

	std::mutex mut;

	size_t findFreeElement()
	{
		size_t result = (~0uL);
		for (size_t i = 0, size = numElements; i < size; ++i)
		{
			if (bits[i] == 0)
			{
				return i;
			}
		}

		return result;
	}

	size_t findNextOrPrevFreeElement(size_t i, size_t maxLoops = 4)
	{
		size_t result = (~0uL);
		size_t size = i + maxLoops;

		if (size >= numElements)
		{
			size = numElements;
		}

		for (size_t j = i; j < size; ++j)
		{
			if (bits[j] == 0)
			{
				return j;
			}
		}

		if (result == (~0uL) && i < numElements)
		{
			size_t prev = 0;

			if (i > maxLoops)
			{
				prev = i - maxLoops;
			}

			for (; prev < i; ++prev)
			{
				if (bits[prev] == 0)
				{
					return prev;
				}
			}
		}

		return result;
	}

public:
	int getNumElements()
	{
		return numElements;
	}

	size_t getFreeElement()
	{
		return firstFreeElement;
	}

	size_t getAddedElementsNum()
	{
		return addedElements;
	}

	poolData_t *get(size_t i)
	{
		if (i < numElements)
		{
			if (bits[i] != 0)
			{
				return &(pool.get()[i]);
			}

			return nullptr;
		}

		return nullptr;
	}

	poolData_t *newElement(size_t *retId = nullptr)
	{
		poolData_t *result = nullptr;
		{
			std::lock_guard<std::mutex> lock(mut);
			if (firstFreeElement != size_t(~0uL) && firstFreeElement < numElements && bits[firstFreeElement] == 0)
			{
				bits[firstFreeElement] = 1;

				result = &(pool.get()[firstFreeElement]);

				if (retId != nullptr)
				{
					*retId = firstFreeElement;
				}
			}
			else
			{
				size_t i = findFreeElement();

				if (i == size_t(~0uL))
				{
					std::cout << "Result is nullptr!!!!!\n";
					std::cout << "Result is nullptr!!!!!\n";
					std::cout << "Result is nullptr!!!!!\n";
					return nullptr;
				}

				bits[i] = 1;

				result = &(pool.get()[i]);

				if (retId != nullptr)
				{
					*retId = i;
				}
			}

			firstFreeElement = findNextOrPrevFreeElement(firstFreeElement);
		}

		if (result != nullptr)
		{
			auto &data = (*result);
			data = std::move(poolData_t());

			++addedElements;
		}
		else
		{
			std::cout << "Result is nullptr!!!!!\n";
			std::cout << "Result is nullptr!!!!!\n";
			std::cout << "Result is nullptr!!!!!\n";
		}

		return result;
	}

	void removeElement(poolData_t *element, bool callDector = true)
	{
		if (element != nullptr)
		{
			if (callDector)
				(*element).~poolData_t();

			uintptr_t ptrstart = reinterpret_cast<uintptr_t>(pool.get());
			uintptr_t ptrpoint = reinterpret_cast<uintptr_t>(element);
			uintptr_t i = (ptrpoint - ptrstart) / sizeof(poolData_t);

			bits[i] = 0;

			if (i < firstFreeElement)
				firstFreeElement = i;

			--addedElements;
		}
	}

	void removeElementByIndex(size_t i, bool callDector = true)
	{
		poolData_t *element = get(i);

		if (element != nullptr)
		{
			if (callDector)
				(*element).~poolData_t();

			bits[i] = 0;

			if (i < firstFreeElement)
				firstFreeElement = i;

			--addedElements;
		}
	}

	CMultiThreadPool()
	{
		pool = std::unique_ptr<poolData_t[]>(new poolData_t[numElements]);
		firstFreeElement = 0;
		addedElements = 0;

		/*for (size_t i = 0, size = numElements; i < size; ++i)
		{
			bits[i] = 0;
		}*/
	}
};

#endif
