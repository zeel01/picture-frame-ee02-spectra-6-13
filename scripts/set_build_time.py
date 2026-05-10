import time

Import("env")

env.Append(
	BUILD_FLAGS=[f"-DBUILD_UNIX_EPOCH={int(time.time())}"]
)
