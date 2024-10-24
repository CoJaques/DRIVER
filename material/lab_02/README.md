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

## Comparaison des méthodes `read()`, `poll()` et `select()` pour la gestion des interruptions dans les systèmes embarqués

Dans cet exemple, nous utilisons trois approches différentes pour attendre des interruptions d'un périphérique UIO (User-space I/O) en espace utilisateur : `read()`, `poll()`, et `select()`. Chaque méthode présente des avantages et des inconvénients en fonction des besoins du programme. Voici un chapitre comparatif de ces trois approches, avec les extraits de code correspondants.

### 1. Utilisation de `read()`

#### Code :
```c
nb = read(fd, &info, sizeof(info));
if (nb == (ssize_t)sizeof(info)) {
    button_state = *(mem_ptr + INTERRUPT_EDGE_BUTTON);
    int16_t value = read_switches(switch_register);
    counter = process_counter(counter, value, button_state);
    set_7_segment(segment1_register, segment2_register, abs(counter));
    set_leds(led_register, counter < 0);

    // clear interrupt
    *(mem_ptr + INTERRUPT_EDGE_BUTTON) = 0xF;
}
```

#### Explication :

- **Fonctionnement** : La fonction `read()` est bloquante. Elle attend qu'une interruption soit générée avant de retourner les données. Une fois que l'interruption est reçue, le traitement de l'interruption se fait (ici, en mettant à jour l'affichage 7 segments et les LEDs).
  
- **Avantages** :
  - Simplicité d'utilisation. Le code est simple et lisible.

- **Inconvénients** :
  - Ne permet pas de gérer plusieurs sources de fichiers à la fois (pas de gestion simultanée de plusieurs descripteurs de fichiers).
  - Bloque l'exécution du programme jusqu'à ce qu'une interruption se produise, ce qui peut poser problème si le programme a besoin de faire d'autres tâches en parallèle.

### 2. Utilisation de `poll()`

#### Code :
```c
struct pollfd fds = {
    .fd = fd,
    .events = POLLIN,
};

int poll_result = poll(&fds, 1, -1);
if (poll_result > 0) {
    nb = read(fd, &info, sizeof(info));
    if (nb == (ssize_t)sizeof(info)) {
        button_state = *(mem_ptr + INTERRUPT_EDGE_BUTTON);
        int16_t value = read_switches(switch_register);
        counter = process_counter(counter, value, button_state);
        set_7_segment(segment1_register, segment2_register, abs(counter));
        set_leds(led_register, counter < 0);

        // clear interrupt
        *(mem_ptr + INTERRUPT_EDGE_BUTTON) = 0xF;
    }
} else {
    perror("poll() failed");
    close(fd);
    exit(EXIT_FAILURE);
}
```

#### Explication :

- **Fonctionnement** : La fonction `poll()` attend un événement sur le descripteur de fichier (`fd`) et ne bloque que si aucun événement ne se produit. Elle permet de surveiller plusieurs descripteurs de fichiers en même temps, bien que dans cet exemple, nous n'en surveillons qu'un seul.

- **Avantages** :
  - Peut gérer plusieurs descripteurs de fichiers simultanément, ce qui est utile dans les applications où plusieurs sources d'interruptions doivent être surveillées.
  - Permet de spécifier un délai d'attente (timeout), offrant plus de contrôle sur le blocage par rapport à `read()`.

- **Inconvénients** :
  - Un peu plus complexe que `read()`.

### 3. Utilisation de `select()`

#### Code :
```c
fd_set read_fds;
FD_ZERO(&read_fds);
FD_SET(fd, &read_fds);

int poll_result = select(fd + 1, &read_fds, NULL, NULL, NULL);
if (poll_result > 0) {
    nb = read(fd, &info, sizeof(info));
    if (nb == (ssize_t)sizeof(info)) {
        button_state = *(mem_ptr + INTERRUPT_EDGE_BUTTON);
        int16_t value = read_switches(switch_register);
        counter = process_counter(counter, value, button_state);
        set_7_segment(segment1_register, segment2_register, abs(counter));
        set_leds(led_register, counter < 0);

        // clear interrupt
        *(mem_ptr + INTERRUPT_EDGE_BUTTON) = 0xF;
    }
} else {
    perror("select() failed");
    close(fd);
    exit(EXIT_FAILURE);
}
```

#### Explication :

- **Fonctionnement** : `select()` est similaire à `poll()`, mais fonctionne en manipulant des ensembles de descripteurs de fichiers à surveiller. Il attend que l'un des descripteurs soit prêt à être lu (dans ce cas, le descripteur `fd`).

- **Avantages** :
  - Comme `poll()`, il permet de surveiller plusieurs descripteurs à la fois.
  
- **Inconvénients** :
  - Moins efficace que `poll()` pour des ensembles de descripteurs de grande taille, car `select()` a une limite sur le nombre maximum de descripteurs qu'il peut surveiller.

Dans tous les cas, il vaut mieux utiliser poll à la place de read car cela permet d'indiquer un timeout et d'éviter d'avoir une solution bloquante. Dans mon cas, j'ai utiliser un timeout infini (-1 pour poll et null pour select) car mon application n'a aucune autre tâche à effectuer et ne fais qu'attendre un appui sur un bouton. L'utilisation de select est obsolète et devrait être remplacé par l'utilisation de poll selon la man-page, cela dû au fait que select limite le nombre de fd max.
