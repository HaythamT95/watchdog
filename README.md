# Watchdog
The Watchdog project offers a solution for ensuring the continuous execution of your process by running a watchdog mechanism. If your process crashes, the watchdog will detect it and automatically re-execute your process, ensuring that it remains running without interruptions.

## Description
The watchdog provided by this project acts as a monitoring mechanism for your process. It constantly keeps track of the execution status of your process. If it detects that your process has crashed, it takes the necessary steps to restart it, ensuring uninterrupted operation.

### Key Features:
**Process Monitoring:** The watchdog continuously monitors the execution of your process to detect any crashes or failures.

**Automatic Recovery:** In the event of a process crash, the watchdog automatically initiates the re-execution of your process, ensuring that it resumes operation without manual intervention.

**Thread Safety:** This function is designed to be used in multi-threaded environments, ensuring safe and reliable operation even in concurrent settings.
