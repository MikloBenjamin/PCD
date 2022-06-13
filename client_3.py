import io
import os
import argparse
import threading
import socket
import math
import glob
import time
import concurrent.futures

HOST = "127.0.0.1"
PORT = 7366
BUFF_MAX_SIZE = 1024
PACKET_SIZE = 1021

NR_IMAGES_AND_OP = 0
CONFIRMATION = 1
NUMBER_OF_PACKETS = 2
PACKET = 3
ERROR = 4

confirmation_semaphore = threading.Semaphore(0) # semafor de confirmare pachete
image_semaphore = threading.Semaphore(0) # semafor pentru pachete de imagini

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

# functie pentru scrierea afisarilor intr-un fisier
log_file = None
def log(text):
    log_file.write(text + "\n")

#  documentatia comenzii
def commandDoc():
    parser = argparse.ArgumentParser(description='With this command you send information to the server about your image processing.')
    parser.add_argument('-fi', type=str, help='a path for a Folder with Image', required=True)
    parser.add_argument('-fpi', type=str, help='a path for a Folder with Processed Image', required=True)
    parser.add_argument('-n', dest='negativ', action='store_true', help='NEGATIV - image will be processing in negativ.')
    parser.add_argument('-s', dest='selia', action='store_true', help='SELIA- image processing option')
    parser.add_argument('-b', dest='blurr', action='store_true', help='BLURR - image processing option')
    parser.add_argument('-w', dest='black_white', action='store_true', help='BLACK & WHITE - image processing option')
    parser.add_argument('-outlog', type=str, help='a path for a LOG file', required=True)

    args = parser.parse_args()
    return args

# verificarea comenzii
def commandAnalyze(argumente):
    options = []
    for arg in vars(argumente):
        value = getattr(argumente, arg)
        if value != False:
            if arg in ["fi"]:
                options.append(value)
            elif arg in ["fpi"]:
                options.append(value)
            elif arg in ["outlog"]:
                global log_file
                log_file = open(value, "w")
            else:
                options.append(arg)
    if len(options) !=  3:
        return 1, "", ""
    elif len(options) == 2:
        return 2, "", ""
    elif len(options) == 0:
        return 3, "", ""
    else:
        if options[2] == "negativ":
            op = 0
        elif options[2] == "selia":
            op = 1
        elif options[2] == "blurr":
            op = 2
        else:
            op = 3
        return 0, options[0], options[1], op

# decodificarea numarului de imagini
def getNumberOfImagePackets(buffer):
    number_of_packets = 0xff & buffer[1]
    number_of_packets |= (0xff & buffer[2]) << 8

    return number_of_packets

# functie pentru convertare in bytes
def convertInBytes(number):
    number_in_bytes1 = number & 0xff
    number_in_bytes2 = (number >> 8) & 0xff

    return number_in_bytes1, number_in_bytes2

# functie pentru creare director
def createFolder(path):
    if not os.path.exists(path):
        os.mkdir(path)
        log(f"Directory for processed images '{path}' is created.")

# firul de executie care trimite informatiile catre server 
def sendImages():
    createFolder(pimage_folder_path)

    for img_index in range(len(list_of_images)):
        img = list_of_images[img_index]
        image = None
        try:
            image = open(img, "rb")
        except FileNotFoundError as error:
            log(error)
            continue

        # calcularea numarul pachetelor si convertarea acesteia in bytes
        image.seek(0, io.SEEK_END)
        nr_of_packets =  math.ceil(image.tell() / PACKET_SIZE)
        image.seek(0, io.SEEK_SET)
        nr_pack_in_bytes1, nr_pack_in_bytes2 = convertInBytes(nr_of_packets)

        log(f"Sending image: {img}")

        s.send(bytearray([NUMBER_OF_PACKETS, nr_pack_in_bytes1, nr_pack_in_bytes2]))

        # asteapta confirmare
        confirmation_semaphore.acquire()

        # inceperea trimiterii pachetelor cu parti din imagine
        while nr_of_packets > 0:
            image_bytes_read = image.read(PACKET_SIZE)
            image_packet_size = len(image_bytes_read)
            packet_size_in_bytes1, packet_size_in_bytes2 = convertInBytes(image_packet_size)
            
            s.send(bytearray([PACKET, packet_size_in_bytes1, packet_size_in_bytes2]) + image_bytes_read)
            
            if nr_of_packets > 1:
               # asteapta confirmare
                confirmation_semaphore.acquire()

            nr_of_packets -= 1
        image.close()
        if img_index < (len(list_of_images) - 1):
            image_semaphore.acquire()
            log("### CAN SEND NEXT IMEAGE ###")

        
# firul de executie care primeste informatii de la server 
def recieveImages():
    global nr_of_image

    option_str = ["NEGATIV", "SEPIA", "BLURR", "BLACK&WHITE"]
    number_of_packets_from_server = -1
    name_counter = 0
    new_file = None
    while nr_of_image > 0:
        # primeste un mesaj
        packet_from_server = s.recv(BUFF_MAX_SIZE)
        if len(packet_from_server) <= 0:
            break

        # caz de confirmare
        if packet_from_server[0] == CONFIRMATION:
            # print(f"Recieve: confirmation_semaphore 1before - {confirmation_semaphore._value}")
            confirmation_semaphore.release()
            # print(f"Recieve: confirmation_semaphore 1after - {confirmation_semaphore._value}")
            continue

        # caz de pachet cu numar de parti de imagini 
        elif packet_from_server[0] == NUMBER_OF_PACKETS:
            log(f"recieveImages: I recieved packet with type {NUMBER_OF_PACKETS}")
            number_of_packets_from_server = getNumberOfImagePackets(packet_from_server) + 1
            createFolder(f"{pimage_folder_path}/{option_str[option]}")
            new_file = open(f"{pimage_folder_path}/{option_str[option]}/{list_of_image_names[name_counter]}", "wb")
            name_counter += 1 
        
        # caz de pachet cu bucata de imagine
        elif packet_from_server[0] == PACKET:
            new_file.write(packet_from_server[PACKET:])

        # decrementam numarul de pachete pentru la final sa nu mai trimita confirmare
        if number_of_packets_from_server > 0:
            number_of_packets_from_server -= 1
            s.send(bytearray([CONFIRMATION]))

        # daca nu mai sunt pachete inseamna ca putem salva imaginea (daca aceasta exista)
        if number_of_packets_from_server == 0:
            if new_file is not None:
                number_of_packets_from_server = -1
                nr_of_image -= 1
                log(f"recieveImages: Image '{list_of_image_names[name_counter - 1]}' SAVED! Saved in: {pimage_folder_path}{str(option)}\n ")
                new_file.close()
                new_file = None
                # print(f"Recieve: image_semaphore 2before - {image_semaphore._value}")
                image_semaphore.release()
                # print(f"Recieve: image_semaphore 2after - {image_semaphore._value}")


if __name__ == "__main__":
    arguments = commandDoc()
    correct, image_folder_path, pimage_folder_path, option = commandAnalyze(arguments)
    if correct == 1:
        log("Client_3: Expect exact 3 arguments.")
    elif correct == 2:
        log("Client_3: Wrong image processing option.")
    elif correct == 3:
        log("Client_3: Need image folder path.")
    else:
        s.connect((HOST, PORT))
        
        # se creaza lista cu imagini
        list_of_image_names = [name for name in os.listdir(image_folder_path) if name.endswith(".png")]
        list_of_images = glob.glob(f"{image_folder_path}/*.png")
        nr_of_image = len(list_of_images)

        # se converteste numarul de imagini, optiounea si pachet descriptorul in bytes pentru a trimi catre server
        nr_image_bytes1, nr_image_bytes2 = convertInBytes(nr_of_image)
        option_bytes1, option_bytes2 = convertInBytes(option)

        buffer = bytearray([NR_IMAGES_AND_OP, nr_image_bytes1, nr_image_bytes2, option_bytes1, option_bytes2])
        s.send(buffer) 
        
        # se creaza firele de executie pentru trimitere poze si primire poze
        thread_send_images = threading.Thread(target=sendImages)
        thread_recieve_processed_images = threading.Thread(target=recieveImages)
        
        thread_send_images.start()
        thread_recieve_processed_images.start()
        
        thread_send_images.join()
        thread_recieve_processed_images.join()

        s.close()
    log("End client")
               