from scapy.all import Ether, IP, TCP, wrpcap
import random

NUM_PACKETS = 10000  # تعداد بسته‌ها در هر سناریو
MAC_SRC = "00:00:00:00:00:01"
MAC_DST = "00:00:00:00:00:02"

# سناریوی ۱: Pure Miss (ترافیک کاملاً ناشناخته)
# فرض: رول‌های OVS شما برای رنج 10.x.x.x تنظیم شده‌اند. ما رنج 192.168.x.x تولید می‌کنیم تا قطعا Miss شود.
def generate_pure_miss(filename="pure_miss.pcap"):
    packets = []
    for _ in range(NUM_PACKETS):
        src_ip = f"192.168.{random.randint(1, 254)}.{random.randint(1, 254)}"
        dst_ip = f"192.168.{random.randint(1, 254)}.{random.randint(1, 254)}"
        pkt = Ether(src=MAC_SRC, dst=MAC_DST) / IP(src=src_ip, dst=dst_ip) / TCP(dport=80, sport=random.randint(1024, 65535))
        packets.append(pkt)
    wrpcap(filename, packets)
    print(f"[*] {filename} generated with {NUM_PACKETS} miss packets.")

# سناریوی ۲: Needle in a Haystack (مچ شدن در آخرین Subtable)
# فرض: رولی با کمترین اولویت و دقیق‌ترین مچ (Exact Match) در OVS برای IP زیر تعریف کرده‌اید.
def generate_needle(target_ip="10.100.100.100", filename="needle.pcap"):
    packets = []
    for _ in range(NUM_PACKETS):
        # آدرس مبدا رندوم، اما آدرس مقصد دقیقا همانی است که در جدول آخر مچ می‌شود
        src_ip = f"10.0.{random.randint(1, 254)}.{random.randint(1, 254)}"
        pkt = Ether(src=MAC_SRC, dst=MAC_DST) / IP(src=src_ip, dst=target_ip) / TCP(dport=80)
        packets.append(pkt)
    wrpcap(filename, packets)
    print(f"[*] {filename} generated. All packets target {target_ip}.")

# سناریوی ۳: Mixed Workload (تحلیل نقطه سر به سر)
# ترکیب Hit و Miss با نسبت مشخص (مثلا ۲۰ درصد Hit و ۸۰ درصد Miss)
def generate_mixed(hit_ratio=0.2, hit_ip="10.0.0.1", filename="mixed_20_80.pcap"):
    packets = []
    hit_count = int(NUM_PACKETS * hit_ratio)
    miss_count = NUM_PACKETS - hit_count
    
    # تولید بسته‌های Hit
    for _ in range(hit_count):
        src_ip = f"10.0.{random.randint(1, 254)}.{random.randint(1, 254)}"
        pkt = Ether(src=MAC_SRC, dst=MAC_DST) / IP(src=src_ip, dst=hit_ip) / TCP(dport=80)
        packets.append(pkt)
        
    # تولید بسته‌های Miss
    for _ in range(miss_count):
        src_ip = f"192.168.{random.randint(1, 254)}.{random.randint(1, 254)}"
        dst_ip = f"192.168.{random.randint(1, 254)}.{random.randint(1, 254)}"
        pkt = Ether(src=MAC_SRC, dst=MAC_DST) / IP(src=src_ip, dst=dst_ip) / TCP(dport=80)
        packets.append(pkt)
        
    # در هم ریختن (Shuffle) ترافیک برای شبیه‌سازی دنیای واقعی
    random.shuffle(packets)
    wrpcap(filename, packets)
    print(f"[*] {filename} generated. Hit: {hit_count}, Miss: {miss_count}.")

if __name__ == "__main__":
    generate_pure_miss()
    generate_needle()
    
    # تولید سه حالت ترکیبی برای پیدا کردن Break-even Point
    generate_mixed(hit_ratio=0.2, filename="mixed_20hit_80miss.pcap")
    generate_mixed(hit_ratio=0.5, filename="mixed_50hit_50miss.pcap")
    generate_mixed(hit_ratio=0.8, filename="mixed_80hit_20miss.pcap")
