/*
 * expandfs - estende la prima partizione della microSD a tutta la scheda.
 *
 * Busybox e' compilato senza fdisk (CONFIG_FDISK non e' impostato), quindi la
 * tabella delle partizioni va riscritta qui. Il programma NON formatta e NON
 * copia niente: si limita a cambiare la lunghezza della partizione 1 nell'MBR
 * e a far rileggere la tabella al kernel. Il resto lo fa rcS.
 *
 * Uso:  expandfs [-n] <disco>            es. expandfs /dev/mmcblk0
 *       -n   dice solo cosa farebbe, senza scrivere
 *
 * Uscita: 0 = partizione estesa (il chiamante deve formattare)
 *         1 = niente da fare (gia' estesa, o scheda piu' piccola)
 *         2 = errore
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <linux/fs.h>

#define SECTOR_SIZE       512u
#define MBR_PART1_OFFSET  0x1BE
#define MBR_SIG_OFFSET    0x1FE

/* Il PM vuole FAT32 con il limite dei 32 GB: oltre, lo spazio resta inusato. */
#define MAX_SECTORS       (32ull * 1024 * 1024 * 1024 / SECTOR_SIZE)

/* Sotto questa soglia non ha senso muoversi: sono i 79 MB dell'immagine. */
#define MIN_GROWTH_SECT   (16ull * 1024 * 1024 / SECTOR_SIZE)

static uint32_t get_le32(const unsigned char* p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
	       ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void put_le32(unsigned char* p, uint32_t v)
{
	p[0] = (unsigned char)(v         & 0xff);
	p[1] = (unsigned char)((v >>  8) & 0xff);
	p[2] = (unsigned char)((v >> 16) & 0xff);
	p[3] = (unsigned char)((v >> 24) & 0xff);
}

int main(int argc, char** argv)
{
	int dry_run = 0;
	const char* device;
	int fd;
	unsigned char mbr[SECTOR_SIZE];
	unsigned char* part;
	uint64_t disk_bytes = 0, disk_sect, want_sect, limit_sect;
	uint32_t start_lba, cur_sect;
	unsigned char type;
	struct stat st;
	int is_file;

	if (argc == 3 && strcmp(argv[1], "-n") == 0) {
		dry_run = 1;
		device = argv[2];
	} else if (argc == 2) {
		device = argv[1];
	} else {
		fprintf(stderr, "uso: expandfs [-n] <disco>\n");
		return 2;
	}

	fd = open(device, dry_run ? O_RDONLY : O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "expandfs: %s: %s\n", device, strerror(errno));
		return 2;
	}

	if (read(fd, mbr, SECTOR_SIZE) != (ssize_t)SECTOR_SIZE) {
		fprintf(stderr, "expandfs: lettura MBR fallita\n");
		close(fd);
		return 2;
	}

	if (mbr[MBR_SIG_OFFSET] != 0x55 || mbr[MBR_SIG_OFFSET + 1] != 0xAA) {
		fprintf(stderr, "expandfs: firma MBR assente, non tocco niente\n");
		close(fd);
		return 2;
	}

	part      = mbr + MBR_PART1_OFFSET;
	type      = part[4];
	start_lba = get_le32(part + 8);
	cur_sect  = get_le32(part + 12);

	/* Solo FAT: 0x0B/0x0C sono FAT32, 0x0E e' FAT16 LBA, 0x06 FAT16.
	   Se trovo altro, la scheda non e' la nostra e mi fermo. */
	if (type != 0x0B && type != 0x0C && type != 0x0E && type != 0x06) {
		fprintf(stderr, "expandfs: partizione 1 di tipo 0x%02x, non FAT: non tocco niente\n", type);
		close(fd);
		return 2;
	}

	if (start_lba == 0 || cur_sect == 0) {
		fprintf(stderr, "expandfs: partizione 1 vuota o malformata\n");
		close(fd);
		return 2;
	}

	/* Su un file normale gli ioctl dei dischi non esistono: la dimensione si
	   prende dal file. Serve per collaudare il programma su un'immagine, e
	   sul dispositivo vero questo ramo non viene mai preso. */
	is_file = (fstat(fd, &st) == 0 && S_ISREG(st.st_mode));

	if (is_file) {
		disk_bytes = (uint64_t)st.st_size;
	} else if (ioctl(fd, BLKGETSIZE64, &disk_bytes) != 0 || disk_bytes == 0) {
		fprintf(stderr, "expandfs: BLKGETSIZE64: %s\n", strerror(errno));
		close(fd);
		return 2;
	}

	if (disk_bytes == 0) {
		fprintf(stderr, "expandfs: dimensione nulla\n");
		close(fd);
		return 2;
	}

	disk_sect  = disk_bytes / SECTOR_SIZE;
	limit_sect = disk_sect < MAX_SECTORS ? disk_sect : MAX_SECTORS;

	if (limit_sect <= start_lba) {
		fprintf(stderr, "expandfs: scheda troppo piccola\n");
		close(fd);
		return 2;
	}

	want_sect = limit_sect - start_lba;

	printf("expandfs: scheda %llu MB, partizione 1 da %u a %llu settori\n",
	       (unsigned long long)(disk_bytes / (1024 * 1024)),
	       cur_sect, (unsigned long long)want_sect);

	if (want_sect <= (uint64_t)cur_sect + MIN_GROWTH_SECT) {
		printf("expandfs: niente da guadagnare, lascio com'e'\n");
		close(fd);
		return 1;
	}

	if (dry_run) {
		close(fd);
		return 0;
	}

	put_le32(part + 12, (uint32_t)want_sect);

	/* I campi CHS non servono piu' a nessuno ma vanno messi nella forma
	   "usa l'LBA", altrimenti qualche strumento si insospettisce. */
	part[5] = 0xFE;
	part[6] = 0xFF;
	part[7] = 0xFF;

	if (lseek(fd, 0, SEEK_SET) != 0 ||
	    write(fd, mbr, SECTOR_SIZE) != (ssize_t)SECTOR_SIZE) {
		fprintf(stderr, "expandfs: scrittura MBR fallita: %s\n", strerror(errno));
		close(fd);
		return 2;
	}

	if (fsync(fd) != 0) {
		fprintf(stderr, "expandfs: fsync fallito: %s\n", strerror(errno));
		close(fd);
		return 2;
	}

	/* Rilettura della tabella: senza questa il kernel continua a vedere la
	   partizione vecchia e mkfs.vfat formatterebbe la dimensione sbagliata. */
	if (!is_file && ioctl(fd, BLKRRPART, 0) != 0) {
		fprintf(stderr, "expandfs: BLKRRPART: %s\n", strerror(errno));
		close(fd);
		return 2;
	}

	close(fd);
	printf("expandfs: fatto\n");
	return 0;
}
