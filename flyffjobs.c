#include "flyfflib.c"

int main(int argc, char *argv[]) {
  Flyff *flyff;
  JobProp *jobs;
  int numJobs, i;

  if (argc != 2) {
    fprintf(stderr, "usage: %s /path/to/flyff\n", argv[0]);
    return 1;
  }

  flyff = MkFlyff(argv[1]);
  jobs = LdJobProps(flyff, &numJobs);
  for (i = 0; i < numJobs; ++i) {
    printf("\n# %s\n", jobs[i].id);
    printf("Attack Speed: %f\n", jobs[i].aspd);
    printf("Max HP factor: %f\n", jobs[i].factorMaxHP);
    printf("Max MP factor: %f\n", jobs[i].factorMaxMP);
    printf("Max FP factor: %f\n", jobs[i].factorMaxFP);
    printf("Def factor %f\n", jobs[i].factorDef);
    printf("HP recovery factor: %f\n", jobs[i].factorHPRecovery);
    printf("MP recovery factor: %f\n", jobs[i].factorMPRecovery);
    printf("FP recovery factor: %f\n", jobs[i].factorFPRecovery);
    printf("sword attack factor: %f\n", jobs[i].meleeSwd);
    printf("axe attack factor: %f\n", jobs[i].meleeAxe);
    printf("staff attack factor: %f\n", jobs[i].meleeStaff);
    printf("stick attack factor: %f\n", jobs[i].meleeStick);
    printf("knuckle attack factor: %f\n", jobs[i].meleeKnuckle);
    printf("wand attack factor: %f\n", jobs[i].meleeWand);
    printf("yoyo attack factor: %f\n", jobs[i].meleeYOYO);
    printf("blocking: %f\n", jobs[i].blocking);
    printf("critical: %f\n", jobs[i].critical);
    printf("icon: %s\n", jobs[i].icon);
  }
  RmJobProps(jobs, numJobs);
  RmFlyff(flyff);

  return 0;
}
