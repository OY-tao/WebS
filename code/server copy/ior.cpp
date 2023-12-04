#include <iostream>
#include <liburing.h>

int main() {
    struct io_uring ring;
    if (io_uring_queue_init(32, &ring, 0) < 0) {
        std::cerr << "io_uring initialization failed." << std::endl;
        return 1;
    }

    int fd = open("example.txt", O_RDONLY);
    if (fd < 0) {
        std::cerr << "Failed to open file." << std::endl;
        return 1;
    }

    struct io_uring_sqe* sqe = io_uring_get_sqe(&ring);
    io_uring_prep_read(sqe, fd, nullptr, 0, 0);
    io_uring_submit(&ring);

    struct io_uring_cqe* cqe;
    if (io_uring_wait_cqe(&ring, &cqe) < 0) {
        std::cerr << "io_uring_wait_cqe failed." << std::endl;
        return 1;
    }

    if (cqe->res < 0) {
        std::cerr << "Async read error." << std::endl;
        return 1;
    }

    std::cout << "Async read succeeded." << std::endl;

    io_uring_cqe_seen(&ring, cqe);
    io_uring_queue_exit(&ring);

    close(fd);

    return 0;
}
