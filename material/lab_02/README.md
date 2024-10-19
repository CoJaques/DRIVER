#### Colin Jaques - Labo 2 DRV

# Exercice 2
## Pourquoi une région de 4096 bytes et non pas 5000 ou 10000 ? Et pourquoi on a spécifié cette adresse ?

La région de 4096 bytes s'explique par le fait que l'on est obligé d'avoir une adresse alignée avec mmap. Sachant qu'une page fait 4Ko sur notre système, cela explique le fait que l'on alloue au moins 1 page.

Allouer 5000 ou 10000 bytes résulterait en un mapping non aligné et donc probablement des erreurs.

L'adresse sélectionnée correspond à l'adresse à laquelle se trouve les PIO permettant de gérer les leds, 7segments switchs etc.

## Quelles sont les différences dans le comportement de mmap() susmentionnées ?
La principale différence réside dans le fait que mmap utilisé avec UIO permet de s'assurer que la zone mémoire accessible est bien limitée à ce qui est défini dans le device tree alors qu'avec mmap utilisé sur /dev/mem ne prends pas du tout en compte la sécurité d'accès.

L'un des autres avantages de UIO est qu'il est possible de gérer les interruptions via le kernel. Sans cela on serait obligé de faire du polling sur les I/O.

A noter aussi que /dev/mem nécessite que l'application conaisse l'adresse mémoire exacte lors de la compilation.

Toutes ces informations ont été trouvées ici : [ici](https://tuxengineering.com/blog/2020/08/15/Linux-Userspace.html)
