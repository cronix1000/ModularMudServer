#pragma once
struct SkillWindupComponent {
        float timeLeft;
        int skillID;   // The script knows what to do
        int targetID;  // The script knows who to hit/heal
};