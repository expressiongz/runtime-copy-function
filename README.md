# runtime-copy-function
Copies functions at runtime and fixes their relative call addresses.
# Explanation
Let's say you want to copy a function at runtime, you may think of just copy and pasting the function by looping through it, finding the end, and then just allocating memory for it and boom std::memcpy comes to make it easy for you.

Well, this would be a good idea, if functions used absolute address calls every time they want to call a function, which isn't the case.

Most CALLs and JMPs you see, take relative addresses as operands. That means there's a calculation that occurs in order to make the JMP / CALL which binds it to be relative to the current location in memory of the IP ( instruction pointer ).

Today I'll be going over that calculation and I'll be showing you how to properly copy a function.

The CALL / JMP formula goes as following: target absolute address - address of the instruction after the CALL instruction.

Seems simple right? That's because it is.

Now, you may see why this becomes an issue when copy and pasting functions, that same relative address is never changed, which means when we try to call a copied function that had relative JMPs or CALLs in it ( which is quite often, I'd say around 95-99% of the time lol )

without fixing the relative addresses, we're guaranteed to crash.

Here's my implementation of how to recalculate and fix relative addresses when copying functions with commented code to further explain
